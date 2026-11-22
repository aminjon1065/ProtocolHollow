#pragma once

#include "CoreMinimal.h"

class ACharacter;

/** Where a gadget laid by a player goes (pulse mine, ritual ward): on the floor below its feet. */
namespace HollowGadgetPlacement
{
	/**
	 * The floor found at most TraceDistance below the character's feet (visibility trace from its middle, ignoring
	 * it), else its feet; yawed like the character.
	 */
	HOLLOWGAME_API FTransform OnFloorBelow(const ACharacter& Placer, float TraceDistance);
}
