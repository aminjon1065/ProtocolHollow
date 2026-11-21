#include "Extraction/HollowExtractionResolver.h"

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
		const bool bKept = bKeepAll || (Slot.bInsured && Catalog.IsInsurable(Slot.Item));
		AddStack(bKept ? Delta.Kept : Delta.Lost, Slot.Item, Slot.Count - Spent);
	}
	if (KeepsLoot(Outcome))
	{
		for (const FHollowItemStack& Stack : Context.Loot)
		{
			AddStack(Delta.Sold, Stack.Item, Stack.Count);
			Delta.Credits += Catalog.GetValue(Stack.Item) * FMath::Max(Stack.Count, 0);
		}
	}
	return Delta;
}
