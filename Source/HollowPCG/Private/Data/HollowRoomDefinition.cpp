#include "Data/HollowRoomDefinition.h"

#include "Data/HollowLevelMetrics.h"
#include "Misc/PackageName.h"
#include "Rules/HollowRoomRules.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowRoomDefinition)

#define LOCTEXT_NAMESPACE "HollowRoomDefinition"

const FPrimaryAssetType UHollowRoomDefinition::AssetType(TEXT("HollowRoom"));

FPrimaryAssetId UHollowRoomDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(AssetType, GetFName());
}

#if WITH_EDITOR
EDataValidationResult UHollowRoomDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	for (const FHollowRoomIssue& Issue : HollowRoomRules::ValidateRoom(*this, *GetDefault<UHollowLevelMetrics>()))
	{
		Context.AddError(HollowRoomRules::Describe(Issue));
		Result = EDataValidationResult::Invalid;
	}
	if (!RoomLevel.IsNull() && !FPackageName::DoesPackageExist(RoomLevel.GetLongPackageName()))
	{
		Context.AddError(FText::Format(LOCTEXT("MissingLevel", "RoomLevel {0} does not exist."), FText::FromString(RoomLevel.GetLongPackageName())));
		Result = EDataValidationResult::Invalid;
	}
	return Result == EDataValidationResult::NotValidated ? EDataValidationResult::Valid : Result;
}
#endif

#undef LOCTEXT_NAMESPACE
