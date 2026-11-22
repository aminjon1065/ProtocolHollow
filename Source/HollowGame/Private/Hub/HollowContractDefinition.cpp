#include "Hub/HollowContractDefinition.h"

#include "Engine/AssetManager.h"
#include "Items/HollowItemDefinition.h"
#include "Sector/HollowSectorFeatureSubsystem.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowContractDefinition)

#define LOCTEXT_NAMESPACE "HollowContractDefinition"

const FPrimaryAssetType UHollowContractDefinition::AssetType(TEXT("HollowContract"));

FPrimaryAssetId UHollowContractDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(AssetType, GetFName());
}

bool UHollowContractDefinition::Exists(const FPrimaryAssetId& Id)
{
	return Id.PrimaryAssetType == AssetType && UAssetManager::IsInitialized() && UAssetManager::Get().GetPrimaryAssetPath(Id).IsValid();
}

const UHollowContractDefinition* UHollowContractDefinition::Load(const FPrimaryAssetId& Id)
{
	return Exists(Id) ? Cast<UHollowContractDefinition>(UAssetManager::Get().GetPrimaryAssetPath(Id).TryLoad()) : nullptr;
}

#if WITH_EDITOR
EDataValidationResult UHollowContractDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (DisplayName.IsEmpty() || SectorName.IsEmpty() || SectorFeature.IsEmpty())
	{
		Context.AddError(FText::Format(LOCTEXT("Contract", "{0}: a contract needs a name, a sector name and a sector Game Feature."), FText::FromName(GetFName())));
		Result = EDataValidationResult::Invalid;
	}
	else if (!UHollowSectorFeatureSubsystem::GetSectorBiome(SectorFeature).IsValid() || UHollowSectorFeatureSubsystem::GetSectorRooms(SectorFeature).IsEmpty())
	{
		Context.AddError(FText::Format(LOCTEXT("Sector", "{0}: sector {1} is not a sector Game Feature with a biome and rooms (ADR-0012)."),
			FText::FromName(GetFName()), FText::FromString(SectorFeature)));
		Result = EDataValidationResult::Invalid;
	}
	if (RewardItems.IsEmpty())
	{
		Context.AddError(FText::Format(LOCTEXT("NoReward", "{0}: a contract names what it pays (RewardItems)."), FText::FromName(GetFName())));
		Result = EDataValidationResult::Invalid;
	}
	for (const FPrimaryAssetId& Item : RewardItems)
	{
		if (Item.PrimaryAssetType != UHollowItemDefinition::AssetType || !UAssetManager::Get().GetPrimaryAssetPath(Item).IsValid())
		{
			Context.AddError(FText::Format(LOCTEXT("Reward", "{0}: reward {1} is not an item."), FText::FromName(GetFName()), FText::FromString(Item.ToString())));
			Result = EDataValidationResult::Invalid;
		}
	}
	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
