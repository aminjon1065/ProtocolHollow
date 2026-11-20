#include "Sector/HollowSectorTopologySubsystem.h"

#include "Engine/World.h"
#include "HollowCoreLog.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowSectorTopologySubsystem)

UHollowSectorTopologySubsystem* UHollowSectorTopologySubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UHollowSectorTopologySubsystem>() : nullptr;
}

bool UHollowSectorTopologySubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UHollowSectorTopologySubsystem::SetTopology(const FHollowSectorTopology& InTopology)
{
	Topology = InTopology;
	UE_LOG(LogHollowCore, Log, TEXT("Sector topology: %d rooms, %d portals, %d vents"), Topology.Rooms.Num(), Topology.Portals.Num(), Topology.Vents.Num());
	OnTopologyChanged.Broadcast();
}
