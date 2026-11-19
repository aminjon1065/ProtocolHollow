#pragma once

#include "Templates/SubclassOf.h"

class UHollowDoorDefinition;
class UNavArea;
struct FHollowDoorNetState;

namespace HollowDoorNav
{
	/**
	 * Area of a door's nav link for its state (Docs/Architecture/03 sec. 3.7): open, opening and breached doors are
	 * doorways; otherwise Barricaded beats Welded (split by WeldedHighThreshold) beats Locked beats Closed.
	 */
	HOLLOWPHYSICS_API TSubclassOf<UNavArea> AreaFor(const FHollowDoorNetState& State, float Weld, const UHollowDoorDefinition& Definition);
}
