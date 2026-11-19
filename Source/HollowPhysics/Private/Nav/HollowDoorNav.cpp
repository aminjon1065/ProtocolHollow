#include "Nav/HollowDoorNav.h"

#include "Doors/HollowDoorDefinition.h"
#include "Doors/HollowDoorTypes.h"
#include "NavAreas/NavArea.h"

TSubclassOf<UNavArea> HollowDoorNav::AreaFor(const FHollowDoorNetState& State, const float Weld, const UHollowDoorDefinition& Definition)
{
	switch (State.State)
	{
	case EHollowDoorState::Open:
	case EHollowDoorState::Opening:
	case EHollowDoorState::Breached:
		return Definition.AreaDoorway;
	default:
		break;
	}
	if (State.HasFlag(EHollowDoorFlags::Barricaded))
	{
		return Definition.AreaBarricaded;
	}
	if (State.State == EHollowDoorState::Welded)
	{
		return Weld >= Definition.WeldedHighThreshold ? Definition.AreaWeldedHigh : Definition.AreaWeldedLow;
	}
	return State.HasFlag(EHollowDoorFlags::Locked) ? Definition.AreaLocked : Definition.AreaClosed;
}
