#include "Data/HollowBiomeDefinition.h"

#include "Algo/StableSort.h"
#include "Generation/HollowSectorGenerator.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowBiomeDefinition)

const FPrimaryAssetType UHollowBiomeDefinition::AssetType(TEXT("HollowBiome"));

FPrimaryAssetId UHollowBiomeDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(AssetType, GetFName());
}

TArray<int32> UHollowBiomeDefinition::AssignSocketRules(const TConstArrayView<FHollowSocketSlot> Slots) const
{
	TArray<int32> Assigned;
	Assigned.Init(INDEX_NONE, Slots.Num());
	for (int32 RuleIndex = 0; RuleIndex < SocketActors.Num(); ++RuleIndex)
	{
		const FHollowSocketActorRule& Rule = SocketActors[RuleIndex];
		TArray<int32> Free;
		for (int32 Slot = 0; Slot < Slots.Num(); ++Slot)
		{
			if (Assigned[Slot] == INDEX_NONE && Slots[Slot].SocketType == Rule.SocketType)
			{
				Free.Add(Slot);
			}
		}
		if (Rule.MaxPerSector > 0)
		{
			if (Rule.Pick == EHollowSocketPick::FarthestFromSpawn)
			{
				Algo::StableSortBy(Free, [&Slots](const int32 Slot) { return -Slots[Slot].RoomDistance; });
			}
			Free.SetNum(FMath::Min(Free.Num(), Rule.MaxPerSector));
		}
		for (const int32 Slot : Free)
		{
			Assigned[Slot] = RuleIndex;
		}
	}
	return Assigned;
}

TSoftClassPtr<AActor> UHollowBiomeDefinition::FindConnectionActor(const EHollowConnectorKind Kind, const uint64 VariantKey) const
{
	const FHollowConnectionActorRule* Rule = ConnectionActors.FindByPredicate([Kind](const FHollowConnectionActorRule& R) { return R.Kind == Kind; });
	if (!Rule)
	{
		return TSoftClassPtr<AActor>();
	}
	return Rule->Variants.IsEmpty() ? Rule->ActorClass : Rule->Variants[static_cast<int32>(VariantKey % static_cast<uint64>(Rule->Variants.Num()))];
}

TArray<TSoftClassPtr<AActor>> UHollowBiomeDefinition::GetConnectionActorClasses() const
{
	TArray<TSoftClassPtr<AActor>> Classes;
	for (const FHollowConnectionActorRule& Rule : ConnectionActors)
	{
		Classes.AddUnique(Rule.ActorClass);
		for (const TSoftClassPtr<AActor>& Variant : Rule.Variants)
		{
			Classes.AddUnique(Variant);
		}
	}
	return Classes;
}

#if WITH_EDITOR
EDataValidationResult UHollowBiomeDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	for (const FString& Error : FHollowSectorGenerator::ValidateRules(GenRules))
	{
		Context.AddError(FText::FromString(Error));
		Result = EDataValidationResult::Invalid;
	}
	return Result == EDataValidationResult::NotValidated ? EDataValidationResult::Valid : Result;
}
#endif
