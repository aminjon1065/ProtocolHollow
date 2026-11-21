#include "Hub/HollowContractDefinition.h"

#include "Engine/AssetManager.h"

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

#if WITH_EDITOR
EDataValidationResult UHollowContractDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (DisplayName.IsEmpty() || SectorFeature.IsEmpty())
	{
		Context.AddError(FText::Format(LOCTEXT("Contract", "{0}: a contract needs a name and a sector Game Feature."), FText::FromName(GetFName())));
		Result = EDataValidationResult::Invalid;
	}
	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
