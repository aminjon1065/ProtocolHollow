#include "Items/HollowItemDefinition.h"

#include "Tags/HollowGameplayTags.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowItemDefinition)

#define LOCTEXT_NAMESPACE "HollowItemDefinition"

const FPrimaryAssetType UHollowItemDefinition::AssetType(TEXT("HollowItem"));

FPrimaryAssetId UHollowItemDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(AssetType, GetFName());
}

#if WITH_EDITOR
EDataValidationResult UHollowItemDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (!ItemType.MatchesTag(HollowTags::Item_Type) || ItemType == HollowTags::Item_Type.GetTag())
	{
		Context.AddError(LOCTEXT("ItemType", "An item needs one Hollow.Item.Type.* child as its type."));
		Result = EDataValidationResult::Invalid;
	}
	if (DisplayName.IsEmpty())
	{
		Context.AddError(LOCTEXT("DisplayName", "An item needs a display name."));
		Result = EDataValidationResult::Invalid;
	}
	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
