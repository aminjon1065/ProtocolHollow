#include "Doors/HollowDoorDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowDoorDefinition)

const FPrimaryAssetType UHollowDoorDefinition::AssetType(TEXT("HollowDoor"));

FPrimaryAssetId UHollowDoorDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(AssetType, GetFName());
}
