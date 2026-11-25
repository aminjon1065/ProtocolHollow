#include "Extraction/HollowExtractionResolver.h"

#include "Crafting/HollowCraftingRules.h"

#include "Tags/HollowGameplayTags.h"

namespace HollowExtractionPrivate
{
	void AddStack(TArray<FHollowItemStack>& Stacks, const FPrimaryAssetId& Item, const int32 Count)
	{
		if (Count <= 0 || !Item.IsValid())
		{
			return;
		}
		if (FHollowItemStack* Existing = Stacks.FindByPredicate([&Item](const FHollowItemStack& Stack) { return Stack.Item == Item; }))
		{
			Existing->Count += Count;
			return;
		}
		FHollowItemStack& Stack = Stacks.AddDefaulted_GetRef();
		Stack.Item = Item;
		Stack.Count = Count;
	}
}

bool FHollowExtractionResolver::KeepsLoadout(const EHollowRaidOutcome Outcome)
{
	return Outcome == EHollowRaidOutcome::Extracted || Outcome == EHollowRaidOutcome::SignalLost || Outcome == EHollowRaidOutcome::None;
}

bool FHollowExtractionResolver::KeepsLoot(const EHollowRaidOutcome Outcome)
{
	return Outcome == EHollowRaidOutcome::Extracted;
}

FHollowProfileDelta FHollowExtractionResolver::Resolve(const FHollowRaidContext& Context, const EHollowRaidOutcome Outcome,
	const FHollowItemCatalog& Catalog)
{
	using namespace HollowExtractionPrivate;
	FHollowProfileDelta Delta;
	if (Outcome == EHollowRaidOutcome::None)
	{
		return Delta;   // no decision yet: nothing changes
	}
	TMap<FPrimaryAssetId, int32> ToSpend;
	for (const FHollowItemStack& Stack : Context.Spent)
	{
		ToSpend.FindOrAdd(Stack.Item) += FMath::Max(Stack.Count, 0);
	}
	const bool bKeepAll = KeepsLoadout(Outcome);
	for (const FHollowLoadoutSlot& Slot : Context.Loadout)
	{
		int32* Spend = ToSpend.Find(Slot.Item);
		const int32 Spent = Spend ? FMath::Clamp(*Spend, 0, FMath::Max(Slot.Count, 0)) : 0;
		if (Spend)
		{
			*Spend -= Spent;
		}
		AddStack(Delta.Spent, Slot.Item, Spent);
		const bool bSavedByInsurance = !bKeepAll && Slot.bInsured && Catalog.IsInsurable(Slot.Item);
		AddStack(bKeepAll || bSavedByInsurance ? Delta.Kept : Delta.Lost, Slot.Item, Slot.Count - Spent);
		if (bSavedByInsurance)
		{
			AddStack(Delta.InsuranceUsed, Slot.Item, Slot.Count - Spent);
		}
	}
	if (KeepsLoot(Outcome))
	{
		for (const FHollowItemStack& Stack : Context.Loot)
		{
			AddStack(Delta.Sold, Stack.Item, Stack.Count);
			Delta.Credits = static_cast<int32>(FMath::Min<int64>(static_cast<int64>(Delta.Credits) + ItemEarnings(Catalog, Stack.Item, Stack.Count, Context.CoreValuePercent), MAX_int32));
		}
	}
	// GP-01b (ADR-0045): the found loot is the player's own: home into the stash, the rest of it sold; or left behind.
	TArray<FHollowItemStack> Stash = Context.Stash;
	for (const FHollowItemStack& Stack : Context.Found)
	{
		if (!KeepsLoot(Outcome))
		{
			AddStack(Delta.FoundLost, Stack.Item, Stack.Count);
			continue;
		}
		const int32 MaxStack = Catalog.GetMaxStack(Stack.Item);
		const int32 Fits = static_cast<int32>(FMath::Clamp<int64>(HollowCraftingRules::RoomInStash(Stash, Stack.Item, MaxStack, Context.StashSlots), 0, FMath::Max(Stack.Count, 0)));
		if (Fits > 0 && HollowCraftingRules::AddToStash(Stash, Stack.Item, Fits, MaxStack, Context.StashSlots > 0 ? Context.StashSlots : MAX_int32))
		{
			AddStack(Delta.Found, Stack.Item, Fits);
		}
		const int32 Sold = FMath::Max(Stack.Count, 0) - Fits;
		if (Sold > 0)
		{
			AddStack(Delta.FoundSold, Stack.Item, Sold);
			const int64 Price = static_cast<int64>(Catalog.GetValue(Stack.Item)) * FMath::Max(Context.SellPercent, 0) / 100;
			Delta.Credits = static_cast<int32>(FMath::Min<int64>(static_cast<int64>(Delta.Credits) + Price * Sold, MAX_int32));
		}
	}
	return Delta;
}

int32 FHollowExtractionResolver::ItemEarnings(const FHollowItemCatalog& Catalog, const FPrimaryAssetId& Item, const int32 Count, const int32 CoreValuePercent)
{
	constexpr int64 PercentBase = 100;
	const int64 Value = static_cast<int64>(Catalog.GetValue(Item)) * FMath::Max(Count, 0);
	const bool bCore = Catalog.GetItemType(Item).MatchesTag(HollowTags::Item_Type_AetherCore);
	const int64 Earned = bCore ? Value * (PercentBase + FMath::Max(CoreValuePercent, 0)) / PercentBase : Value;
	return static_cast<int32>(FMath::Min<int64>(Earned, MAX_int32));
}
