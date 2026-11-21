#include "Crafting/HollowShipUpgradeDefinition.h"

#include "Crafting/HollowCraftingRules.h"
#include "Engine/AssetManager.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowShipUpgradeDefinition)

#define LOCTEXT_NAMESPACE "HollowShipUpgradeDefinition"

const FPrimaryAssetType UHollowShipUpgradeDefinition::AssetType(TEXT("HollowUpgrade"));

FPrimaryAssetId UHollowShipUpgradeDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(AssetType, GetFName());
}

FHollowUpgradeTerms UHollowShipUpgradeDefinition::ToTerms() const
{
	FHollowUpgradeTerms Terms;
	Terms.Id = GetPrimaryAssetId();
	Terms.Price = Price;
	Terms.ExtraInsuredSlots = ExtraInsuredSlots;
	Terms.CoreValuePercent = CoreValuePercent;
	Terms.RaidItems = RaidItems;
	return Terms;
}

TArray<FHollowUpgradeTerms> UHollowShipUpgradeDefinition::LoadAll()
{
	TArray<FHollowUpgradeTerms> All;
	if (!UAssetManager::IsInitialized())
	{
		return All;
	}
	TArray<FPrimaryAssetId> Ids;
	UAssetManager::Get().GetPrimaryAssetIdList(AssetType, Ids);
	Ids.Sort([](const FPrimaryAssetId& A, const FPrimaryAssetId& B) { return A.ToString() < B.ToString(); });
	for (const FPrimaryAssetId& Id : Ids)
	{
		if (const UHollowShipUpgradeDefinition* Upgrade = Cast<UHollowShipUpgradeDefinition>(UAssetManager::Get().GetPrimaryAssetPath(Id).TryLoad()))
		{
			All.Add(Upgrade->ToTerms());
		}
	}
	return All;
}

TMap<FPrimaryAssetId, FHollowUpgradeTerms> UHollowShipUpgradeDefinition::LoadAllById()
{
	TMap<FPrimaryAssetId, FHollowUpgradeTerms> ById;
	for (FHollowUpgradeTerms& Upgrade : LoadAll())
	{
		const FPrimaryAssetId Id = Upgrade.Id;
		ById.Add(Id, MoveTemp(Upgrade));
	}
	return ById;
}

#if WITH_EDITOR
EDataValidationResult UHollowShipUpgradeDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	bool bValid = !DisplayName.IsEmpty() && Price > 0 && (ExtraInsuredSlots > 0 || CoreValuePercent > 0 || !RaidItems.IsEmpty());
	for (const FHollowItemStack& Item : RaidItems)
	{
		bValid &= Item.Item.IsValid() && Item.Count > 0;
	}
	if (!bValid)
	{
		Context.AddError(FText::Format(LOCTEXT("Upgrade", "{0}: an upgrade needs a name, a price and at least one effect (raid items each an item and a count)."),
			FText::FromName(GetFName())));
		Result = EDataValidationResult::Invalid;
	}
	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
