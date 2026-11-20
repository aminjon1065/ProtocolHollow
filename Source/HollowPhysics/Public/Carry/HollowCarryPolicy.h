#pragma once

#include "Carry/HollowCarryTypes.h"

struct FHollowCarryMovementTuning;

/** The server's state of one carrier, as its movement component keeps it (server only). */
struct FHollowServerCarry
{
	EHollowCarryClaim Current = EHollowCarryClaim::None;
	/** State before the last change: still the floor while a stricter state waits out the grace. */
	EHollowCarryClaim Previous = EHollowCarryClaim::None;
	double ChangedAt = 0.0;
};

/**
 * Carry slowdown rules shared by client prediction and server simulation (ADR-0015, Docs/Architecture/03 sec. 3.5).
 * EffectiveSlowdown = max(Claimed, ServerFloor): claiming to be slower is never a cheat, so the server always accepts
 * it, and a stricter server state only binds after a grace, by when an honest client claims it too. Severity is the
 * speed scale of the mode (lower is slower). Tested by Hollow.Physics.Carry.SlowdownPolicy.
 */
namespace HollowCarryPolicy
{
	/** The slower of two claims; A on a tie. */
	HOLLOWPHYSICS_API EHollowCarryClaim Slower(EHollowCarryClaim A, EHollowCarryClaim B, const FHollowCarryMovementTuning& Tuning);

	/**
	 * The server's lower bound on a carrier's slowdown: a stricter state counts only once SecondsSinceChange reaches
	 * GraceSeconds (inclusive), a gentler one at once. A negative age (clock skew) counts as zero.
	 */
	HOLLOWPHYSICS_API EHollowCarryClaim ServerFloor(const FHollowServerCarry& Server, double SecondsSinceChange, float GraceSeconds,
		const FHollowCarryMovementTuning& Tuning);

	/** The slowdown the server simulates a move with. */
	HOLLOWPHYSICS_API EHollowCarryClaim Effective(EHollowCarryClaim Claimed, const FHollowServerCarry& Server, double SecondsSinceChange,
		float GraceSeconds, const FHollowCarryMovementTuning& Tuning);
}
