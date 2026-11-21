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

	/** At most this many saved moves for one client frame: a frame that long is a hitch, past it the engine clamps as before. */
	inline constexpr int32 MaxMovesPerFrame = 8;

	/**
	 * How many saved moves a client frame of DeltaTime becomes so none is longer than MaxMoveDeltaTime (ADR-0028): the
	 * engine clamps a longer move's simulated time on the client but the server simulates it from the timestamps, which
	 * then run ahead (the first-step correction of ADR-0015). At least 1, at most MaxMoves; 1 without a positive limit.
	 */
	HOLLOWPHYSICS_API int32 MoveSteps(float DeltaTime, float MaxMoveDeltaTime, int32 MaxMoves = MaxMovesPerFrame);
}
