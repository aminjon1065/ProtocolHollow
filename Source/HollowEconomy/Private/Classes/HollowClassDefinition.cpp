#include "Classes/HollowClassDefinition.h"

#include "Crafting/HollowCraftingRules.h"
#include "Engine/AssetManager.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowClassDefinition)

#define LOCTEXT_NAMESPACE "HollowClassDefinition"

const FPrimaryAssetType UHollowClassDefinition::AssetType(TEXT("HollowClass"));

FPrimaryAssetId UHollowClassDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(AssetType, GetFName());
}

FHollowClassTerms UHollowClassDefinition::ToTerms() const
{
	FHollowClassTerms Terms;
	Terms.Id = GetPrimaryAssetId();
	Terms.StartingLoadout = StartingLoadout;
	return Terms;
}

const UHollowClassDefinition* UHollowClassDefinition::Load(const FPrimaryAssetId& Id)
{
	if (Id.PrimaryAssetType != AssetType || !UAssetManager::IsInitialized())
	{
		return nullptr;
	}
	return Cast<UHollowClassDefinition>(UAssetManager::Get().GetPrimaryAssetPath(Id).TryLoad());
}

TArray<FHollowClassTerms> UHollowClassDefinition::LoadAll()
{
	TArray<FHollowClassTerms> All;
	if (!UAssetManager::IsInitialized())
	{
		return All;
	}
	TArray<FPrimaryAssetId> Ids;
	UAssetManager::Get().GetPrimaryAssetIdList(AssetType, Ids);
	Ids.Sort([](const FPrimaryAssetId& A, const FPrimaryAssetId& B) { return A.ToString() < B.ToString(); });
	for (const FPrimaryAssetId& Id : Ids)
	{
		if (const UHollowClassDefinition* Class = Load(Id))
		{
			All.Add(Class->ToTerms());
		}
	}
	return All;
}

#if WITH_EDITOR
EDataValidationResult UHollowClassDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	bool bValid = !DisplayName.IsEmpty() && ClassTag.IsValid() && !AbilitySet.IsNull() && !AttributeEffect.IsNull() && VisorColor.A > 0.f;
	for (const FHollowLoadoutSlot& Slot : StartingLoadout)
	{
		bValid &= Slot.Item.IsValid() && Slot.Count > 0;
	}
	if (!bValid)
	{
		Context.AddError(FText::Format(LOCTEXT("Class", "{0}: a class needs a name, a Hollow.Class tag, an ability set, an attribute effect, a visible visor colour and a starting loadout of items with counts."),
			FText::FromName(GetFName())));
		Result = EDataValidationResult::Invalid;
	}
	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
