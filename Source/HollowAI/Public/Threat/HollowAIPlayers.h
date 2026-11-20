#pragma once

#include "CoreMinimal.h"
#include "Templates/FunctionFwd.h"

class AActor;
class APawn;
class UWorld;

/** Who the AI cares about: living players (ADR-0017, ADR-0019). */
namespace HollowAIPlayers
{
	/** A player-controlled pawn that is not dead (Hollow.State.Dead). */
	HOLLOWAI_API bool IsLivingPlayer(const AActor* Actor);

	/** Every living player's pawn, in controller order. */
	HOLLOWAI_API void ForEach(const UWorld* World, TFunctionRef<void(APawn&)> Visit);
}
