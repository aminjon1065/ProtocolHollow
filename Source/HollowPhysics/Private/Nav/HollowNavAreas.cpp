#include "Nav/HollowNavAreas.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowNavAreas)

// Debug draw colours only (navmesh visualisation): green passable, yellow closed, red/orange blocked for Standard agents.
UHollowNavArea_Doorway::UHollowNavArea_Doorway(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) { DrawColor = FColor::Green; }
UHollowNavArea_DoorClosed::UHollowNavArea_DoorClosed(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) { DrawColor = FColor::Yellow; }
UHollowNavArea_DoorLocked::UHollowNavArea_DoorLocked(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) { DrawColor = FColor::Purple; }
UHollowNavArea_DoorWeldedLow::UHollowNavArea_DoorWeldedLow(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) { DrawColor = FColor::Orange; }
UHollowNavArea_DoorWeldedHigh::UHollowNavArea_DoorWeldedHigh(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) { DrawColor = FColor::Red; }
UHollowNavArea_DoorBarricaded::UHollowNavArea_DoorBarricaded(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) { DrawColor = FColor::Magenta; }
UHollowNavArea_BulkheadIntact::UHollowNavArea_BulkheadIntact(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) { DrawColor = FColor::Black; }
