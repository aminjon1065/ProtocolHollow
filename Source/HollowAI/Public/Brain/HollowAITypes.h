#pragma once

#include "CoreMinimal.h"

#include "HollowAITypes.generated.h"

/**
 * What an enemy is doing (ADR-0017). The StateTree picks one per state; AHollowAIController runs it on timers and
 * movement and ability callbacks (no Tick) and tells the state's task when it is done.
 */
UENUM(BlueprintType)
enum class EHollowAIActivity : uint8
{
	None,
	/** Walk to a random reachable point around home, pause. */
	Patrol,
	/** Close in on the target until it is within engage range. */
	Chase,
	/** Go where the last stimulus was (a noise, the spot the target vanished), look around, forget it. */
	Investigate,
	/** Melee strike at the target (an ability, wind-up telegraphed). */
	Attack,
	/** Get past the obstacle in the way: open it, wait for it, or break it (structural blows). */
	Breach
};
