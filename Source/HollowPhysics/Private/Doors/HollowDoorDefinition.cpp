#include "Doors/HollowDoorDefinition.h"

#include "Nav/HollowNavAreas.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowDoorDefinition)

UHollowDoorDefinition::UHollowDoorDefinition(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// The Hollow door areas are the structural default; a door type may point at its own area classes.
	AreaDoorway = UHollowNavArea_Doorway::StaticClass();
	AreaClosed = UHollowNavArea_DoorClosed::StaticClass();
	AreaLocked = UHollowNavArea_DoorLocked::StaticClass();
	AreaWeldedLow = UHollowNavArea_DoorWeldedLow::StaticClass();
	AreaWeldedHigh = UHollowNavArea_DoorWeldedHigh::StaticClass();
	AreaBarricaded = UHollowNavArea_DoorBarricaded::StaticClass();
}

const FPrimaryAssetType UHollowDoorDefinition::AssetType(TEXT("HollowDoor"));

FPrimaryAssetId UHollowDoorDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(AssetType, GetFName());
}
