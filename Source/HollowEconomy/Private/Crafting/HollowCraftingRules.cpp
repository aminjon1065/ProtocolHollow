#include "Crafting/HollowCraftingRules.h"

#include "Algo/Count.h"
#include "Crafting/HollowCraftingRecipe.h"
#include "Crafting/HollowEconomyConfig.h"
#include "Crafting/HollowShipUpgradeDefinition.h"
#include "Classes/HollowClassDefinition.h"
#include "HollowEconomyLog.h"
#include "Profile/HollowEconomySettings.h"
#include "Profile/HollowProfileTypes.h"
#include "Util/HollowEnumUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowCraftingRules)

namespace HollowCraftingPrivate
{
	constexpr int32 PercentBase = 100;

	FHollowProfileTransaction Make(const EHollowTransactionKind Kind)
	{
		FHollowProfileTransaction Transaction;
		Transaction.Id = FGuid::NewGuid();
		Transaction.Kind = Kind;
		return Transaction;
	}

	/** Credits as a whole number again, never past what an int32 holds. */
	int32 ToCredits(const int64 Value)
	{
		return static_cast<int32>(FMath::Clamp<int64>(Value, 0, MAX_int32));
	}
}

FHollowProfileTransaction FHollowProfileTransaction::Buy(const FPrimaryAssetId& Item, const int32 Count)
{
	FHollowProfileTransaction Transaction = HollowCraftingPrivate::Make(EHollowTransactionKind::Buy);
	Transaction.Item = Item;
	Transaction.Count = Count;
	return Transaction;
}

FHollowProfileTransaction FHollowProfileTransaction::Sell(const FPrimaryAssetId& Item, const int32 Count)
{
	FHollowProfileTransaction Transaction = HollowCraftingPrivate::Make(EHollowTransactionKind::Sell);
	Transaction.Item = Item;
	Transaction.Count = Count;
	return Transaction;
}

FHollowProfileTransaction FHollowProfileTransaction::Craft(const FPrimaryAssetId& Recipe, const int32 Times)
{
	FHollowProfileTransaction Transaction = HollowCraftingPrivate::Make(EHollowTransactionKind::Craft);
	Transaction.Recipe = Recipe;
	Transaction.Count = Times;
	return Transaction;
}

FHollowProfileTransaction FHollowProfileTransaction::Insure(const int32 Slot)
{
	FHollowProfileTransaction Transaction = HollowCraftingPrivate::Make(EHollowTransactionKind::Insure);
	Transaction.Slot = Slot;
	return Transaction;
}

FHollowProfileTransaction FHollowProfileTransaction::Equip(const FPrimaryAssetId& Item, const int32 Count)
{
	FHollowProfileTransaction Transaction = HollowCraftingPrivate::Make(EHollowTransactionKind::Equip);
	Transaction.Item = Item;
	Transaction.Count = Count;
	return Transaction;
}

FHollowProfileTransaction FHollowProfileTransaction::Unequip(const int32 Slot, const int32 Count)
{
	FHollowProfileTransaction Transaction = HollowCraftingPrivate::Make(EHollowTransactionKind::Unequip);
	Transaction.Slot = Slot;
	Transaction.Count = Count;
	return Transaction;
}

FHollowProfileTransaction FHollowProfileTransaction::BuyUpgrade(const FPrimaryAssetId& Upgrade)
{
	FHollowProfileTransaction Transaction = HollowCraftingPrivate::Make(EHollowTransactionKind::BuyUpgrade);
	Transaction.Upgrade = Upgrade;
	return Transaction;
}

FHollowProfileTransaction FHollowProfileTransaction::SelectClass(const FPrimaryAssetId& ClassId)
{
	FHollowProfileTransaction Transaction = HollowCraftingPrivate::Make(EHollowTransactionKind::SelectClass);
	Transaction.ClassId = ClassId;
	return Transaction;
}

FString FHollowProfileTransaction::Describe() const
{
	switch (Kind)
	{
	case EHollowTransactionKind::BuyUpgrade:
		return FString::Printf(TEXT("BuyUpgrade %s"), *Upgrade.PrimaryAssetName.ToString());
	case EHollowTransactionKind::SelectClass:
		return FString::Printf(TEXT("SelectClass %s"), *ClassId.PrimaryAssetName.ToString());
	case EHollowTransactionKind::Craft:
		return FString::Printf(TEXT("Craft %s x%d"), *Recipe.PrimaryAssetName.ToString(), Count);
	case EHollowTransactionKind::Insure:
		return FString::Printf(TEXT("Insure slot %d"), Slot);
	case EHollowTransactionKind::Unequip:
		return FString::Printf(TEXT("Unequip slot %d x%d"), Slot, Count);
	default:
		return FString::Printf(TEXT("%s %s x%d"), *HollowEnumToString(Kind), *Item.PrimaryAssetName.ToString(), Count);
	}
}

FHollowEconomyTerms FHollowEconomyTerms::Load(const FHollowProfileState& State, const FHollowProfileTransaction* Transaction)
{
	const UHollowEconomySettings* Settings = GetDefault<UHollowEconomySettings>();
	FHollowEconomyTerms Terms;
	Terms.MaxLoadoutSlots = Settings->MaxLoadoutSlots;
	Terms.LoadoutItemTypes = Settings->LoadoutItemTypes;
	for (const FHollowEquipSlotLimit& Limit : Settings->EquipSlotLimits)
	{
		Terms.EquipSlotLimits.Add(Limit.Slot, Limit.Limit);
	}
	TArray<FPrimaryAssetId> Items;
	if (const UHollowEconomyConfig* Config = UHollowEconomyConfig::Get())
	{
		Terms.SellPercent = Config->SellPercent;
		Terms.InsurancePercent = Config->InsurancePercent;
		Terms.MaxInsuredSlots = Config->MaxInsuredSlots;
		Terms.StashSlots = Config->StashSlots;
		for (const FHollowShopOffer& Offer : Config->Offers)
		{
			Terms.ShopPrices.Add(Offer.Item, Offer.Price);
			Items.AddUnique(Offer.Item);
		}
	}
	else
	{
		UE_LOG(LogHollowEconomy, Warning, TEXT("No economy config (Hollow Economy settings: EconomyConfig): no shop, no stash."));
	}
	for (FHollowRecipeTerms& Recipe : UHollowCraftingRecipe::LoadAll())
	{
		for (const FHollowItemStack& Input : Recipe.Inputs)
		{
			Items.AddUnique(Input.Item);
		}
		Items.AddUnique(Recipe.Output.Item);
		const FPrimaryAssetId Id = Recipe.Id;
		Terms.Recipes.Add(Id, MoveTemp(Recipe));
	}
	for (FHollowUpgradeTerms& Upgrade : UHollowShipUpgradeDefinition::LoadAll())
	{
		for (const FHollowItemStack& Item : Upgrade.RaidItems)
		{
			Items.AddUnique(Item.Item);
		}
		const FPrimaryAssetId Id = Upgrade.Id;
		Terms.Upgrades.Add(Id, MoveTemp(Upgrade));
	}
	Terms.DefaultClass = Settings->DefaultClass;
	for (FHollowClassTerms& Class : UHollowClassDefinition::LoadAll())
	{
		for (const FHollowLoadoutSlot& Slot : Class.StartingLoadout)
		{
			Items.AddUnique(Slot.Item);
		}
		const FPrimaryAssetId Id = Class.Id;
		Terms.Classes.Add(Id, MoveTemp(Class));
	}
	for (const FHollowItemStack& Stack : State.Profile.Stash)
	{
		Items.AddUnique(Stack.Item);
	}
	for (const FHollowLoadoutSlot& Slot : State.Profile.Loadout)
	{
		Items.AddUnique(Slot.Item);
	}
	if (Transaction && Transaction->Item.IsValid())
	{
		Items.AddUnique(Transaction->Item);
	}
	Terms.Catalog = FHollowItemCatalog::Load(Items);
	return Terms;
}

namespace HollowCraftingRules
{
	EHollowTransactionResult Apply(FHollowProfileState& State, const FHollowProfileTransaction& Transaction, const FHollowEconomyTerms& Terms)
	{
		if (!Transaction.Id.IsValid())
		{
			return EHollowTransactionResult::BadRequest;
		}
		if (State.RecentTransactions.Contains(Transaction.Id))
		{
			return EHollowTransactionResult::AlreadyApplied;
		}
		if (State.Escrow.IsActive())
		{
			return EHollowTransactionResult::InRaid;
		}
		FHollowProfile& Profile = State.Profile;
		EHollowTransactionResult Result = EHollowTransactionResult::BadRequest;
		switch (Transaction.Kind)
		{
		case EHollowTransactionKind::Buy:
			Result = Buy(Profile, Transaction.Item, Transaction.Count, Terms);
			break;
		case EHollowTransactionKind::Sell:
			Result = Sell(Profile, Transaction.Item, Transaction.Count, Terms);
			break;
		case EHollowTransactionKind::Craft:
			Result = Craft(Profile, Transaction.Recipe, Transaction.Count, Terms);
			break;
		case EHollowTransactionKind::Insure:
			Result = Insure(Profile, Transaction.Slot, Terms);
			break;
		case EHollowTransactionKind::Equip:
			Result = Equip(Profile, Transaction.Item, Transaction.Count, Terms);
			break;
		case EHollowTransactionKind::Unequip:
			Result = Unequip(Profile, Transaction.Slot, Transaction.Count, Terms);
			break;
		case EHollowTransactionKind::BuyUpgrade:
			Result = BuyUpgrade(Profile, Transaction.Upgrade, Terms);
			break;
		case EHollowTransactionKind::SelectClass:
			Result = SelectClass(Profile, Transaction.ClassId, Terms);
			break;
		}
		if (Result == EHollowTransactionResult::Applied)
		{
			State.RecentTransactions.Add(Transaction.Id);
			const int32 Excess = State.RecentTransactions.Num() - RememberedTransactions;
			if (Excess > 0)
			{
				State.RecentTransactions.RemoveAt(0, Excess);
			}
		}
		return Result;
	}

	EHollowTransactionResult Buy(FHollowProfile& Profile, const FPrimaryAssetId& Item, const int32 Count, const FHollowEconomyTerms& Terms)
	{
		if (Count <= 0)
		{
			return EHollowTransactionResult::BadRequest;
		}
		if (!Terms.Catalog.Contains(Item))
		{
			return EHollowTransactionResult::UnknownItem;
		}
		const int32* Price = Terms.ShopPrices.Find(Item);
		if (!Price)
		{
			return EHollowTransactionResult::NotForSale;
		}
		const int64 Cost = static_cast<int64>(*Price) * Count;
		if (Cost > Profile.Credits)
		{
			return EHollowTransactionResult::NotEnoughCredits;
		}
		if (!AddToStash(Profile.Stash, Item, Count, Terms.Catalog.GetMaxStack(Item), Terms.StashSlots))
		{
			return EHollowTransactionResult::StashFull;
		}
		Profile.Credits = HollowCraftingPrivate::ToCredits(Profile.Credits - Cost);
		return EHollowTransactionResult::Applied;
	}

	EHollowTransactionResult Sell(FHollowProfile& Profile, const FPrimaryAssetId& Item, const int32 Count, const FHollowEconomyTerms& Terms)
	{
		if (Count <= 0)
		{
			return EHollowTransactionResult::BadRequest;
		}
		if (!Terms.Catalog.Contains(Item))
		{
			return EHollowTransactionResult::UnknownItem;
		}
		if (!TakeFromStash(Profile.Stash, Item, Count))
		{
			return EHollowTransactionResult::NotEnoughItems;
		}
		Profile.Credits = HollowCraftingPrivate::ToCredits(Profile.Credits + static_cast<int64>(SellPrice(Terms, Item)) * Count);
		return EHollowTransactionResult::Applied;
	}

	EHollowTransactionResult Craft(FHollowProfile& Profile, const FPrimaryAssetId& Recipe, const int32 Times, const FHollowEconomyTerms& Terms)
	{
		const FHollowRecipeTerms* Found = Terms.Recipes.Find(Recipe);
		if (!Found)
		{
			return EHollowTransactionResult::UnknownRecipe;
		}
		if (Times <= 0)
		{
			return EHollowTransactionResult::BadRequest;
		}
		bool bKnownItems = Terms.Catalog.Contains(Found->Output.Item);
		for (const FHollowItemStack& Input : Found->Inputs)
		{
			bKnownItems &= Terms.Catalog.Contains(Input.Item);
		}
		if (!bKnownItems)
		{
			return EHollowTransactionResult::UnknownItem;
		}
		const int64 Cost = static_cast<int64>(Found->Credits) * Times;
		if (Cost > Profile.Credits)
		{
			return EHollowTransactionResult::NotEnoughCredits;
		}
		TArray<FHollowItemStack> Stash = Profile.Stash;
		for (const FHollowItemStack& Input : Found->Inputs)
		{
			if (!TakeFromStash(Stash, Input.Item, Input.Count * Times))
			{
				return EHollowTransactionResult::NotEnoughItems;
			}
		}
		const FHollowItemStack& Output = Found->Output;
		if (!AddToStash(Stash, Output.Item, Output.Count * Times, Terms.Catalog.GetMaxStack(Output.Item), Terms.StashSlots))
		{
			return EHollowTransactionResult::StashFull;
		}
		Profile.Stash = MoveTemp(Stash);
		Profile.Credits = HollowCraftingPrivate::ToCredits(Profile.Credits - Cost);
		return EHollowTransactionResult::Applied;
	}

	EHollowTransactionResult Insure(FHollowProfile& Profile, const int32 Slot, const FHollowEconomyTerms& Terms)
	{
		if (!Profile.Loadout.IsValidIndex(Slot))
		{
			return EHollowTransactionResult::BadRequest;
		}
		FHollowLoadoutSlot& Target = Profile.Loadout[Slot];
		if (!Terms.Catalog.Contains(Target.Item))
		{
			return EHollowTransactionResult::UnknownItem;
		}
		if (!Terms.Catalog.IsInsurable(Target.Item))
		{
			return EHollowTransactionResult::NotInsurable;
		}
		if (Target.bInsured)
		{
			return EHollowTransactionResult::AlreadyInsured;
		}
		const int32 MaxInsured = Terms.MaxInsuredSlots + SumEffects(Profile.Upgrades, Terms.Upgrades).ExtraInsuredSlots;
		if (Algo::CountIf(Profile.Loadout, [](const FHollowLoadoutSlot& S) { return S.bInsured; }) >= MaxInsured)
		{
			return EHollowTransactionResult::InsuranceLimit;
		}
		const int32 Price = InsurancePrice(Terms, Target);
		if (Price > Profile.Credits)
		{
			return EHollowTransactionResult::NotEnoughCredits;
		}
		Target.bInsured = true;
		Profile.Credits -= Price;
		return EHollowTransactionResult::Applied;
	}

	EHollowTransactionResult Equip(FHollowProfile& Profile, const FPrimaryAssetId& Item, const int32 Count, const FHollowEconomyTerms& Terms)
	{
		if (Count <= 0)
		{
			return EHollowTransactionResult::BadRequest;
		}
		if (!Terms.Catalog.Contains(Item))
		{
			return EHollowTransactionResult::UnknownItem;
		}
		if (!Terms.Catalog.GetItemType(Item).MatchesAny(Terms.LoadoutItemTypes))
		{
			return EHollowTransactionResult::NotForRaids;
		}
		TArray<FPrimaryAssetId> Held;
		for (const FHollowLoadoutSlot& Slot : Profile.Loadout)
		{
			Held.Add(Slot.Item);
		}
		const TMap<FGameplayTag, int32> Limits = EquipSlotLimits(Terms.EquipSlotLimits, SumEffects(Profile.Upgrades, Terms.Upgrades));
		const bool bOneOf = Terms.Catalog.GetEquipSlot(Item).IsValid() && Terms.Catalog.GetMaxStack(Item) <= 1;
		if (!CanTakeIntoSlot(Held, Item, Terms.Catalog, Limits) || (bOneOf && Count > 1))
		{
			return EHollowTransactionResult::SlotTaken;
		}
		TArray<FHollowItemStack> Stash = Profile.Stash;
		if (!TakeFromStash(Stash, Item, Count))
		{
			return EHollowTransactionResult::NotEnoughItems;
		}
		const int32 MaxStack = Terms.Catalog.GetMaxStack(Item);
		TArray<FHollowLoadoutSlot> Loadout = Profile.Loadout;
		int32 Left = Count;
		for (FHollowLoadoutSlot& Slot : Loadout)
		{
			if (Left > 0 && Slot.Item == Item && !Slot.bInsured && Slot.Count < MaxStack)
			{
				const int32 Added = FMath::Min(Left, MaxStack - Slot.Count);
				Slot.Count += Added;
				Left -= Added;
			}
		}
		while (Left > 0)
		{
			if (Loadout.Num() >= Terms.MaxLoadoutSlots)
			{
				return EHollowTransactionResult::LoadoutFull;
			}
			FHollowLoadoutSlot& Slot = Loadout.AddDefaulted_GetRef();
			Slot.Item = Item;
			Slot.Count = FMath::Min(Left, MaxStack);
			Left -= Slot.Count;
		}
		Profile.Stash = MoveTemp(Stash);
		Profile.Loadout = MoveTemp(Loadout);
		return EHollowTransactionResult::Applied;
	}

	EHollowTransactionResult Unequip(FHollowProfile& Profile, const int32 Slot, const int32 Count, const FHollowEconomyTerms& Terms)
	{
		if (!Profile.Loadout.IsValidIndex(Slot) || Count < 0)
		{
			return EHollowTransactionResult::BadRequest;
		}
		const FHollowLoadoutSlot& Source = Profile.Loadout[Slot];
		const int32 Taken = Count == 0 ? Source.Count : Count;
		if (!Terms.Catalog.Contains(Source.Item))
		{
			return EHollowTransactionResult::UnknownItem;
		}
		if (Taken > Source.Count)
		{
			return EHollowTransactionResult::NotEnoughItems;
		}
		if (!AddToStash(Profile.Stash, Source.Item, Taken, Terms.Catalog.GetMaxStack(Source.Item), Terms.StashSlots))
		{
			return EHollowTransactionResult::StashFull;
		}
		Profile.Loadout[Slot].Count -= Taken;
		if (Profile.Loadout[Slot].Count <= 0)
		{
			Profile.Loadout.RemoveAt(Slot);   // the slot goes, with its insurance
		}
		return EHollowTransactionResult::Applied;
	}

	EHollowTransactionResult BuyUpgrade(FHollowProfile& Profile, const FPrimaryAssetId& Upgrade, const FHollowEconomyTerms& Terms)
	{
		const FHollowUpgradeTerms* Found = Terms.Upgrades.Find(Upgrade);
		if (!Found)
		{
			return EHollowTransactionResult::UnknownUpgrade;
		}
		if (Profile.Upgrades.Contains(Upgrade))
		{
			return EHollowTransactionResult::AlreadyOwned;
		}
		if (Found->Price > Profile.Credits)
		{
			return EHollowTransactionResult::NotEnoughCredits;
		}
		Profile.Credits -= Found->Price;
		Profile.Upgrades.Add(Upgrade);
		return EHollowTransactionResult::Applied;
	}

	FPrimaryAssetId CurrentClass(const FHollowProfile& Profile, const FHollowEconomyTerms& Terms)
	{
		return Profile.ClassId.IsValid() ? Profile.ClassId : Terms.DefaultClass;
	}

	EHollowTransactionResult SelectClass(FHollowProfile& Profile, const FPrimaryAssetId& ClassId, const FHollowEconomyTerms& Terms)
	{
		const FHollowClassTerms* NewClass = Terms.Classes.Find(ClassId);
		if (!NewClass)
		{
			return EHollowTransactionResult::UnknownClass;
		}
		const FPrimaryAssetId Current = CurrentClass(Profile, Terms);
		if (ClassId == Current)
		{
			return EHollowTransactionResult::AlreadySelected;
		}
		if (!Current.IsValid())
		{
			return EHollowTransactionResult::BadRequest;   // no class to keep the loadout for (no default class set)
		}
		// The new class's own loadout, or its starting loadout the first time; the current one is kept for its class.
		const int32 Stored = Profile.ClassLoadouts.IndexOfByPredicate([&ClassId](const FHollowClassLoadout& Each) { return Each.ClassId == ClassId; });
		TArray<FHollowLoadoutSlot> Next = Stored != INDEX_NONE ? MoveTemp(Profile.ClassLoadouts[Stored].Loadout) : NewClass->StartingLoadout;
		if (Stored != INDEX_NONE)
		{
			Profile.ClassLoadouts.RemoveAt(Stored);
		}
		FHollowClassLoadout& Kept = Profile.ClassLoadouts.AddDefaulted_GetRef();
		Kept.ClassId = Current;
		Kept.Loadout = MoveTemp(Profile.Loadout);
		Profile.Loadout = MoveTemp(Next);
		Profile.ClassId = ClassId;
		return EHollowTransactionResult::Applied;
	}

	TMap<FGameplayTag, int32> EquipSlotLimits(const TMap<FGameplayTag, int32>& Base, const FHollowUpgradeEffects& Ship)
	{
		TMap<FGameplayTag, int32> Limits = Base;
		for (const TPair<FGameplayTag, int32>& Extra : Ship.ExtraEquipSlots)
		{
			if (int32* Limit = Limits.Find(Extra.Key))
			{
				*Limit += Extra.Value;   // a slot without a limit stays without one
			}
		}
		return Limits;
	}

	bool CanTakeIntoSlot(const TConstArrayView<FPrimaryAssetId> Loadout, const FPrimaryAssetId& Item, const FHollowItemCatalog& Catalog,
		const TMap<FGameplayTag, int32>& Limits)
	{
		const FGameplayTag Slot = Catalog.GetEquipSlot(Item);
		const int32* Limit = Slot.IsValid() ? Limits.Find(Slot) : nullptr;
		if (Loadout.Contains(Item))
		{
			return !Slot.IsValid() || Catalog.GetMaxStack(Item) > 1;   // more of it: only if it stacks (a second relic or tool does nothing)
		}
		if (!Limit)
		{
			return true;
		}
		TArray<FPrimaryAssetId, TInlineAllocator<8>> Different;
		for (const FPrimaryAssetId& Held : Loadout)
		{
			if (Catalog.GetEquipSlot(Held) == Slot)
			{
				Different.AddUnique(Held);
			}
		}
		return Different.Num() < *Limit;
	}

	FHollowUpgradeEffects SumEffects(const TConstArrayView<FPrimaryAssetId> Owned, const TMap<FPrimaryAssetId, FHollowUpgradeTerms>& Upgrades)
	{
		FHollowUpgradeEffects Effects;
		TArray<FPrimaryAssetId, TInlineAllocator<8>> Counted;
		for (const FPrimaryAssetId& Id : Owned)
		{
			const FHollowUpgradeTerms* Upgrade = Upgrades.Find(Id);
			if (!Upgrade || Counted.Contains(Id))
			{
				continue;
			}
			Counted.Add(Id);
			Effects.ExtraInsuredSlots += FMath::Max(Upgrade->ExtraInsuredSlots, 0);
			Effects.CoreValuePercent += FMath::Max(Upgrade->CoreValuePercent, 0);
			for (const FHollowEquipSlotLimit& Extra : Upgrade->ExtraEquipSlots)
			{
				Effects.ExtraEquipSlots.FindOrAdd(Extra.Slot) += FMath::Max(Extra.Limit, 0);
			}
			for (const FHollowItemStack& Item : Upgrade->RaidItems)
			{
				if (FHollowItemStack* Existing = Effects.RaidItems.FindByPredicate([&Item](const FHollowItemStack& S) { return S.Item == Item.Item; }))
				{
					Existing->Count += Item.Count;
				}
				else
				{
					Effects.RaidItems.Add(Item);
				}
			}
		}
		return Effects;
	}

	int32 SellPrice(const FHollowEconomyTerms& Terms, const FPrimaryAssetId& Item)
	{
		return Terms.Catalog.GetValue(Item) * Terms.SellPercent / HollowCraftingPrivate::PercentBase;
	}

	int32 InsurancePrice(const FHollowEconomyTerms& Terms, const FHollowLoadoutSlot& Slot)
	{
		const int64 Insured = static_cast<int64>(Terms.Catalog.GetValue(Slot.Item)) * FMath::Max(Slot.Count, 0) * Terms.InsurancePercent;
		return HollowCraftingPrivate::ToCredits((Insured + HollowCraftingPrivate::PercentBase - 1) / HollowCraftingPrivate::PercentBase);
	}

	bool AddToStash(TArray<FHollowItemStack>& Stash, const FPrimaryAssetId& Item, const int32 Count, const int32 MaxStack, const int32 StashSlots)
	{
		if (Count <= 0)
		{
			return Count == 0;
		}
		if (MaxStack <= 0 || !Item.IsValid())
		{
			return false;
		}
		TArray<FHollowItemStack> Next = Stash;
		int32 Left = Count;
		for (FHollowItemStack& Stack : Next)
		{
			if (Left > 0 && Stack.Item == Item && Stack.Count < MaxStack)
			{
				const int32 Added = FMath::Min(Left, MaxStack - Stack.Count);
				Stack.Count += Added;
				Left -= Added;
			}
		}
		while (Left > 0)
		{
			if (Next.Num() >= StashSlots)
			{
				return false;
			}
			FHollowItemStack& Stack = Next.AddDefaulted_GetRef();
			Stack.Item = Item;
			Stack.Count = FMath::Min(Left, MaxStack);
			Left -= Stack.Count;
		}
		Stash = MoveTemp(Next);
		return true;
	}

	int64 RoomInStash(const TConstArrayView<FHollowItemStack> Stash, const FPrimaryAssetId& Item, const int32 MaxStack, const int32 StashSlots)
	{
		if (MaxStack <= 0 || !Item.IsValid())
		{
			return 0;
		}
		if (StashSlots <= 0)
		{
			return MAX_int32;
		}
		int64 Room = 0;
		for (const FHollowItemStack& Stack : Stash)
		{
			Room += Stack.Item == Item ? FMath::Max(MaxStack - Stack.Count, 0) : 0;
		}
		return Room + static_cast<int64>(FMath::Max(StashSlots - Stash.Num(), 0)) * MaxStack;
	}

	bool TakeFromStash(TArray<FHollowItemStack>& Stash, const FPrimaryAssetId& Item, const int32 Count)
	{
		if (Count < 0 || CountOf(Stash, Item) < Count)
		{
			return false;
		}
		int32 Left = Count;
		for (int32 Index = Stash.Num() - 1; Index >= 0 && Left > 0; --Index)
		{
			FHollowItemStack& Stack = Stash[Index];
			if (Stack.Item == Item)
			{
				const int32 Taken = FMath::Min(Left, Stack.Count);
				Stack.Count -= Taken;
				Left -= Taken;
			}
		}
		Stash.RemoveAll([](const FHollowItemStack& Stack) { return Stack.Count <= 0; });
		return true;
	}

	int32 CountOf(const TConstArrayView<FHollowItemStack> Stacks, const FPrimaryAssetId& Item)
	{
		int32 Count = 0;
		for (const FHollowItemStack& Stack : Stacks)
		{
			Count += Stack.Item == Item ? FMath::Max(Stack.Count, 0) : 0;
		}
		return Count;
	}
}
