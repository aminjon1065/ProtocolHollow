#include "Loot/HollowLootTable.h"

#include "Math/RandomStream.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowLootTable)

#define LOCTEXT_NAMESPACE "HollowLootTable"

TArray<FHollowItemStack> UHollowLootTableDefinition::Roll(const int32 Seed) const
{
	return HollowLootRules::Roll(Rows, MinDrops, MaxDrops, Seed);
}

#if WITH_EDITOR
EDataValidationResult UHollowLootTableDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (MaxDrops < MinDrops || (MaxDrops > 0 && Rows.IsEmpty()))
	{
		Context.AddError(FText::Format(LOCTEXT("Drops", "{0}: MaxDrops is below MinDrops, or there is nothing to roll."), FText::FromName(GetFName())));
		Result = EDataValidationResult::Invalid;
	}
	for (const FHollowLootRow& Row : Rows)
	{
		if (!Row.Item.IsValid() || Row.MaxCount < Row.MinCount)
		{
			Context.AddError(FText::Format(LOCTEXT("Row", "{0}: a row without an item or with MaxCount below MinCount."), FText::FromName(GetFName())));
			Result = EDataValidationResult::Invalid;
		}
	}
	return Result;
}
#endif

namespace HollowLootRules
{
	TArray<FHollowItemStack> Roll(const TConstArrayView<FHollowLootRow> Rows, const int32 MinDrops, const int32 MaxDrops, const int32 Seed)
	{
		TArray<FHollowItemStack> Stacks;
		int64 TotalWeight = 0;
		for (const FHollowLootRow& Row : Rows)
		{
			TotalWeight += Row.Item.IsValid() ? FMath::Max(Row.Weight, 0) : 0;
		}
		if (TotalWeight <= 0 || MaxDrops <= 0)
		{
			return Stacks;
		}
		FRandomStream Stream(Seed);
		const int32 Drops = Stream.RandRange(FMath::Max(MinDrops, 0), FMath::Max(MinDrops, MaxDrops));
		for (int32 Drop = 0; Drop < Drops; ++Drop)
		{
			int64 Pick = Stream.RandRange(0, static_cast<int32>(FMath::Min<int64>(TotalWeight, MAX_int32)) - 1);
			for (const FHollowLootRow& Row : Rows)
			{
				const int32 Weight = Row.Item.IsValid() ? FMath::Max(Row.Weight, 0) : 0;
				if (Pick >= Weight)
				{
					Pick -= Weight;
					continue;
				}
				const int32 Count = Stream.RandRange(FMath::Max(Row.MinCount, 1), FMath::Max(Row.MinCount, Row.MaxCount));
				if (FHollowItemStack* Existing = Stacks.FindByPredicate([&Row](const FHollowItemStack& Stack) { return Stack.Item == Row.Item; }))
				{
					Existing->Count += Count;
				}
				else
				{
					FHollowItemStack& Stack = Stacks.AddDefaulted_GetRef();
					Stack.Item = Row.Item;
					Stack.Count = Count;
				}
				break;
			}
		}
		return Stacks;
	}

	int32 FoundHeld(const int32 Held, const int32 Taken, const int32 Found)
	{
		return FMath::Clamp(Held - FMath::Max(Taken, 0), 0, FMath::Max(Found, 0));
	}
}

#undef LOCTEXT_NAMESPACE
