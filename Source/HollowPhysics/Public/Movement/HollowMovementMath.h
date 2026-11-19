#pragma once

#include "CoreMinimal.h"

struct FHollowGroundSpeedTuning;

/** Pure movement rules shared by client prediction and server simulation (tested by Hollow.Physics.Movement.*). */
namespace HollowMovementMath
{
	/**
	 * True if a sprint request may be honoured: sprint enabled in tuning, not crouching, and moving
	 * within SprintMaxAngle of the facing direction. Directions are projected onto the ground plane.
	 */
	HOLLOWPHYSICS_API bool CanSprint(const FHollowGroundSpeedTuning& Tuning, bool bWantsToSprint, bool bIsCrouching,
		const FVector& MoveDirection, const FVector& FacingDirection);

	/** Multiplier for the walking max speed: SprintSpeedMultiplier while CanSprint, otherwise 1. */
	HOLLOWPHYSICS_API float GetSpeedMultiplier(const FHollowGroundSpeedTuning& Tuning, bool bWantsToSprint, bool bIsCrouching,
		const FVector& MoveDirection, const FVector& FacingDirection);
}
