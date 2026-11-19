#include "Data/HollowToolDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowToolDefinition)

const FPrimaryAssetType UHollowToolDefinition::AssetType(TEXT("HollowTool"));

FPrimaryAssetId UHollowToolDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(AssetType, GetFName());
}
