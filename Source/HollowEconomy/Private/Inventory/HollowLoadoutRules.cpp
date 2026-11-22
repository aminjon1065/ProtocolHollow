#include "Inventory/HollowLoadoutRules.h"

#include "Crafting/HollowCraftingRules.h"
#include "Crafting/HollowShipUpgradeDefinition.h"
#include "Profile/HollowEconomySettings.h"
#include "Util/HollowEnumUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowLoadoutRules)

namespace HollowLoadoutRules
{
	TArray<FHollowItemStack> Validate(const TConstArrayView<FHollowItemStack> Slots, const FHollowItemCatalog& Catalog,
		const FGameplayTagContainer& RaidTypes, const int32 MaxSlots, TArray<FHollowLoadoutIssue>* OutIssues)
	{
		return Validate(Slots, Catalog, RaidTypes, MaxSlots, TMap<FGameplayTag, int32>(), OutIssues);
	}

	TArray<FHollowItemStack> Validate(const TConstArrayView<FHollowItemStack> Slots, const FHollowItemCatalog& Catalog,
		const FGameplayTagContainer& RaidTypes, const int32 MaxSlots, const TMap<FGameplayTag, int32>& EquipLimits, TArray<FHollowLoadoutIssue>* OutIssues)
	{
		const auto Report = [OutIssues](const FPrimaryAssetId& Item, const EHollowLoadoutIssue Issue)
		{
			if (OutIssues)
			{
				OutIssues->Add({Item, Issue});
			}
		};
		TArray<FHollowItemStack> Accepted;
		TArray<FPrimaryAssetId> AcceptedItems;   // one entry per accepted slot, for the equip slot rule
		int32 AcceptedSlots = 0;
		for (const FHollowItemStack& Slot : Slots)
		{
			if (!Catalog.Contains(Slot.Item))
			{
				Report(Slot.Item, EHollowLoadoutIssue::UnknownItem);
				continue;
			}
			if (!Catalog.GetItemType(Slot.Item).MatchesAny(RaidTypes))
			{
				Report(Slot.Item, EHollowLoadoutIssue::NotForRaids);
				continue;
			}
			if (Slot.Count <= 0)
			{
				Report(Slot.Item, EHollowLoadoutIssue::NoCount);
				continue;
			}
			if (AcceptedSlots >= MaxSlots)
			{
				Report(Slot.Item, EHollowLoadoutIssue::TooManySlots);
				continue;
			}
			if (!HollowCraftingRules::CanTakeIntoSlot(AcceptedItems, Slot.Item, Catalog, EquipLimits))
			{
				Report(Slot.Item, EHollowLoadoutIssue::SlotTaken);
				continue;
			}
			AcceptedItems.Add(Slot.Item);
			const int32 MaxStack = Catalog.GetMaxStack(Slot.Item);
			if (Slot.Count > MaxStack)
			{
				Report(Slot.Item, EHollowLoadoutIssue::OverStack);
			}
			++AcceptedSlots;
			const int32 Count = FMath::Min(Slot.Count, MaxStack);
			if (FHollowItemStack* Existing = Accepted.FindByPredicate([&Slot](const FHollowItemStack& Stack) { return Stack.Item == Slot.Item; }))
			{
				Existing->Count += Count;
			}
			else
			{
				FHollowItemStack& Stack = Accepted.AddDefaulted_GetRef();
				Stack.Item = Slot.Item;
				Stack.Count = Count;
			}
		}
		return Accepted;
	}

	TArray<FHollowItemStack> ValidateForRaid(const TConstArrayView<FHollowItemStack> Slots, const TConstArrayView<FPrimaryAssetId> Upgrades,
		TArray<FHollowLoadoutIssue>* OutIssues)
	{
		TArray<FPrimaryAssetId> Items;
		for (const FHollowItemStack& Slot : Slots)
		{
			Items.AddUnique(Slot.Item);
		}
		const UHollowEconomySettings* Settings = GetDefault<UHollowEconomySettings>();
		TMap<FGameplayTag, int32> Base;
		for (const FHollowEquipSlotLimit& Limit : Settings->EquipSlotLimits)
		{
			Base.Add(Limit.Slot, Limit.Limit);
		}
		const TMap<FGameplayTag, int32> Limits = HollowCraftingRules::EquipSlotLimits(Base,
			HollowCraftingRules::SumEffects(Upgrades, UHollowShipUpgradeDefinition::LoadAllById()));
		return Validate(Slots, FHollowItemCatalog::Load(Items), Settings->LoadoutItemTypes, Settings->MaxLoadoutSlots, Limits, OutIssues);
	}

	TArray<FHollowItemStack> ToStacks(const TConstArrayView<FHollowLoadoutSlot> Loadout)
	{
		TArray<FHollowItemStack> Stacks;
		Stacks.Reserve(Loadout.Num());
		for (const FHollowLoadoutSlot& Slot : Loadout)
		{
			FHollowItemStack& Stack = Stacks.AddDefaulted_GetRef();
			Stack.Item = Slot.Item;
			Stack.Count = Slot.Count;
		}
		return Stacks;
	}

	FString Describe(const TConstArrayView<FHollowItemStack> Stacks)
	{
		TArray<FString> Parts;
		for (const FHollowItemStack& Stack : Stacks)
		{
			Parts.Add(FString::Printf(TEXT("%s x%d"), *Stack.Item.PrimaryAssetName.ToString(), Stack.Count));
		}
		return Parts.IsEmpty() ? FString(TEXT("none")) : FString::Join(Parts, TEXT(", "));
	}

	FString Describe(const TConstArrayView<FHollowLoadoutIssue> Issues)
	{
		TArray<FString> Parts;
		for (const FHollowLoadoutIssue& Issue : Issues)
		{
			Parts.Add(FString::Printf(TEXT("%s %s"), *Issue.Item.ToString(), *HollowEnumToString(Issue.Issue)));
		}
		return Parts.IsEmpty() ? FString(TEXT("none")) : FString::Join(Parts, TEXT(", "));
	}
}
